//! The semantics and the contracts of every operation, over Mata leaves.

mod automata;
mod common;

use automata::{nft, single, star, tuple};
use common::nfa;
use lazymat::{Error, Formula, Options, Tape, Term};
use mata::nft::{DONT_CARE, Nft};
use mata::{Alphabet, Nfa, Symbol};

const A: Symbol = Symbol::new(b'a' as u32);
const B: Symbol = Symbol::new(b'b' as u32);
const C: Symbol = Symbol::new(b'c' as u32);
const D: Symbol = Symbol::new(b'd' as u32);
const X: Symbol = Symbol::new(b'x' as u32);

/// The symbols of `names` in a Mata alphabet.
fn named<const N: usize>(names: [&str; N]) -> [Symbol; N] {
	let alphabet = Alphabet::from_names(names).expect("valid names");

	names.map(|name| alphabet.symbol(name).expect("a declared name"))
}

fn empty_nfa() -> Nfa {
	nfa(1, &[0], &[], &[])
}

fn epsilon_nfa() -> Nfa {
	nfa(1, &[0], &[0], &[])
}

/// The relation `symbols*`, a loop on one tuple.
fn tuple_star(symbols: &[Symbol]) -> Nft {
	nft(symbols.len(), 1, &[0], &[0], &[(0, symbols, 0)])
}

/// A loop on `(a, b)` with no final state.
fn empty_nft() -> Nft {
	nft(2, 1, &[0], &[], &[(0, &[A, B], 0)])
}

/// Checks whether each term is empty as expected; a failure names the case by its index.
fn assert_emptiness(
	formula: &Formula,
	cases: impl IntoIterator<Item = (Term, bool)>,
) -> Result<(), Error> {
	for (index, (term, expected)) in cases.into_iter().enumerate() {
		assert_eq!(formula.is_empty(term)?, expected, "case {index}");
	}

	Ok(())
}

// Leaf NFA and NFT emptiness

#[test]
fn leaf_emptiness() -> Result<(), Error> {
	let mut formula = Formula::new();
	let cases = [
		(formula.leaf(&empty_nfa())?, true),
		(formula.leaf(&epsilon_nfa())?, false),
		(formula.leaf(&single(A))?, false),
		(formula.leaf(&tuple(&[A, B]))?, false),
		(formula.leaf(&tuple_star(&[A, B]))?, false),
		(formula.leaf(&empty_nft())?, true),
		(formula.leaf(&tuple(&[A, B, C]))?, false),
		// Two initial states, of which only the second is live.
		(formula.leaf(&nfa(4, &[0, 1], &[3], &[(1, A, 3)]))?, false),
		(
			formula.leaf(&nft(2, 3, &[0, 1], &[2], &[(1, &[A, B], 2)]))?,
			false,
		),
	];

	assert_emptiness(&formula, cases)
}

// Boolean NFA combinations

#[test]
fn unions_and_intersections() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let epsilon = formula.leaf(&epsilon_nfa())?;
	let empty = formula.leaf(&empty_nfa())?;
	let another_empty = formula.leaf(&empty_nfa())?;

	let cases = [
		(formula.union(a, b)?, false),
		(formula.union(a, empty)?, false),
		(formula.union(empty, a)?, false),
		(formula.union(empty, another_empty)?, true),
		(formula.intersection(a, b)?, true),
		(formula.intersection(a, epsilon)?, true),
		(formula.intersection(a, empty)?, true),
	];

	assert_emptiness(&formula, cases)
}

#[test]
fn nft_intersection_treats_dont_care_as_a_wildcard() -> Result<(), Error> {
	let mut formula = Formula::new();
	let wildcard = formula.leaf(&tuple(&[DONT_CARE, B]))?;
	let concrete = formula.leaf(&tuple(&[A, B]))?;
	let both = formula.intersection(wildcard, concrete)?;

	assert!(!formula.is_empty(both)?);

	Ok(())
}

// Complement

#[test]
fn complements_of_languages() -> Result<(), Error> {
	let mut formula = Formula::new();
	let empty = formula.leaf(&empty_nfa())?;
	let a = formula.leaf(&single(A))?;
	let a_star = formula.leaf(&star(A))?;
	let not_empty = formula.complement(empty)?;
	let not_a = formula.complement(a)?;
	let not_a_star = formula.complement(a_star)?;

	let cases = [
		(not_empty, false),
		(not_a, false),
		(not_a_star, true),
		(formula.complement(not_empty)?, true),
		(formula.complement(not_a)?, false),
	];
	assert_emptiness(&formula, cases)?;

	// The query alphabet adds b, which the complement of a* holds.
	let over_ab = Options::new().alphabet(*b"ab");
	assert!(!formula.check(not_a_star, &over_ab)?.is_empty());

	Ok(())
}

#[test]
fn de_morgan_complement_of_union() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let epsilon = formula.leaf(&epsilon_nfa())?;
	let union = formula.union(a, epsilon)?;
	let lhs = formula.complement(union)?;
	let not_a = formula.complement(a)?;
	let not_epsilon = formula.complement(epsilon)?;
	let rhs = formula.intersection(not_a, not_epsilon)?;

	assert_eq!(formula.is_empty(lhs)?, formula.is_empty(rhs)?);

	Ok(())
}

#[test]
fn de_morgan_complement_of_intersection() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let intersection = formula.intersection(a, b)?;
	let lhs = formula.complement(intersection)?;
	let not_a = formula.complement(a)?;
	let not_b = formula.complement(b)?;
	let rhs = formula.union(not_a, not_b)?;

	assert_eq!(formula.is_empty(lhs)?, formula.is_empty(rhs)?);
	assert!(!formula.is_empty(lhs)?);

	Ok(())
}

// Images

#[test]
fn images_of_singleton_and_empty_relations() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let empty = formula.leaf(&empty_nfa())?;
	let a_to_b = formula.leaf(&tuple(&[A, B]))?;
	let b_to_a = formula.leaf(&tuple(&[B, A]))?;
	let no_pairs = formula.leaf(&empty_nft())?;

	let cases = [
		(formula.post_image(a, a_to_b)?, false),
		(formula.post_image(b, a_to_b)?, true),
		(formula.post_image(empty, a_to_b)?, true),
		(formula.post_image(a, no_pairs)?, true),
		(formula.pre_image(b, a_to_b)?, false),
		(formula.pre_image(a, a_to_b)?, true),
		(formula.pre_image(a, b_to_a)?, false),
		(formula.pre_image(empty, a_to_b)?, true),
		(formula.pre_image(b, no_pairs)?, true),
	];

	assert_emptiness(&formula, cases)
}

#[test]
fn post_image_respects_separate_per_tape_alphabets() -> Result<(), Error> {
	let [src, dst] = named(["src", "dst"]);
	let mut formula = Formula::new();
	let src_language = formula.leaf(&single(src))?;
	let dst_language = formula.leaf(&single(dst))?;
	let relation = formula.leaf(&tuple(&[src, dst]))?;
	let post = formula.post_image(src_language, relation)?;
	let both = formula.intersection(post, dst_language)?;
	let wrong = formula.intersection(post, src_language)?;

	assert!(!formula.is_empty(post)?);
	assert!(!formula.is_empty(both)?);
	assert!(formula.is_empty(wrong)?);

	Ok(())
}

#[test]
fn images_of_epsilon() -> Result<(), Error> {
	let mut formula = Formula::new();
	let epsilon = formula.leaf(&epsilon_nfa())?;
	let single = formula.leaf(&tuple(&[A, B]))?;
	let looping = formula.leaf(&tuple_star(&[A, B]))?;

	let cases = [
		(formula.post_image(epsilon, single)?, true),
		(formula.pre_image(epsilon, single)?, true),
		(formula.post_image(epsilon, looping)?, false),
		(formula.pre_image(epsilon, looping)?, false),
	];

	assert_emptiness(&formula, cases)
}

// Tape alphabets

#[test]
fn post_image_uses_only_the_output_alphabet() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a_star = formula.leaf(&star(A))?;
	let a = formula.leaf(&single(A))?;
	let relation = formula.leaf(&tuple_star(&[A, B]))?;
	let post = formula.post_image(a_star, relation)?;
	let not_post = formula.complement(post)?;
	let with_a = formula.intersection(post, a)?;

	assert!(!formula.is_empty(post)?);
	assert!(formula.is_empty(not_post)?);
	assert!(formula.is_empty(with_a)?);

	Ok(())
}

#[test]
fn pre_image_uses_only_the_input_alphabet() -> Result<(), Error> {
	let mut formula = Formula::new();
	let b_star = formula.leaf(&star(B))?;
	let relation = formula.leaf(&tuple_star(&[A, B]))?;
	let pre = formula.pre_image(b_star, relation)?;
	let not_pre = formula.complement(pre)?;

	assert!(!formula.is_empty(pre)?);
	assert!(formula.is_empty(not_pre)?);

	Ok(())
}

// Relation complement

#[test]
fn complement_of_universal_relation_is_empty() -> Result<(), Error> {
	let mut formula = Formula::new();
	let relation = formula.leaf(&tuple_star(&[A, B]))?;
	let not_relation = formula.complement(relation)?;

	assert!(!formula.is_empty(relation)?);
	assert!(formula.is_empty(not_relation)?);

	let over_abc = Options::new().alphabet(*b"abc");
	assert!(!formula.check(not_relation, &over_abc)?.is_empty());

	Ok(())
}

#[test]
fn tape_alphabets_define_the_complement_tape_by_tape() -> Result<(), Error> {
	let [input, output, other_output] = named(["in", "out", "other_out"]);
	let mut formula = Formula::new();
	let relation = formula.leaf(&tuple_star(&[input, output]))?;
	let not_relation = formula.complement(relation)?;

	let options = Options::new()
		.tape_alphabet(0, [input])
		.tape_alphabet(1, [output]);
	assert!(formula.check(not_relation, &options)?.is_empty());

	let options = options.tape_alphabet(1, [other_output]);
	assert!(!formula.check(not_relation, &options)?.is_empty());

	Ok(())
}

#[test]
fn relation_complements() -> Result<(), Error> {
	let mut formula = Formula::new();
	let relation = formula.leaf(&tuple(&[A, B]))?;
	let empty = formula.leaf(&empty_nft())?;
	let not_relation = formula.complement(relation)?;
	let not_empty = formula.complement(empty)?;
	let not_not_relation = formula.complement(not_relation)?;
	let not_not_empty = formula.complement(not_empty)?;

	assert!(!formula.is_empty(not_not_relation)?);
	assert!(!formula.is_empty(not_empty)?);
	assert!(formula.is_empty(not_not_empty)?);

	Ok(())
}

// Composition, identity, projection, synchronized products

#[test]
fn composition_chains_two_singleton_relations() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let c = formula.leaf(&single(C))?;
	let ab = formula.leaf(&tuple(&[A, B]))?;
	let bc = formula.leaf(&tuple(&[B, C]))?;
	let ac = formula.compose(ab, bc)?;
	let ca = formula.compose(bc, ab)?;

	let cases = [
		(formula.post_image(a, ac)?, false),
		(formula.pre_image(c, ac)?, false),
		(formula.pre_image(a, ac)?, true),
		(formula.post_image(a, ca)?, true),
	];

	assert_emptiness(&formula, cases)
}

#[test]
fn composition_with_mismatched_sync_symbol_is_empty() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let ab = formula.leaf(&tuple(&[A, B]))?;
	let cd = formula.leaf(&tuple(&[C, D]))?;
	let composed = formula.compose(ab, cd)?;
	let post = formula.post_image(a, composed)?;

	assert!(formula.is_empty(composed)?);
	assert!(formula.is_empty(post)?);

	Ok(())
}

#[test]
fn composition_matches_dont_care_on_synchronized_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let c = formula.leaf(&single(C))?;
	let left = formula.leaf(&tuple(&[A, DONT_CARE]))?;
	let right = formula.leaf(&tuple(&[B, C]))?;
	let composed = formula.compose(left, right)?;
	let post = formula.post_image(a, composed)?;
	let post_c = formula.intersection(post, c)?;

	assert!(!formula.is_empty(composed)?);
	assert!(!formula.is_empty(post_c)?);

	Ok(())
}

#[test]
fn identity_maps_a_language_to_the_diagonal() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let diagonal = formula.identity(a)?;
	let post_a = formula.post_image(a, diagonal)?;
	let post_b = formula.post_image(b, diagonal)?;

	assert!(!formula.is_empty(diagonal)?);
	assert!(!formula.is_empty(post_a)?);
	assert!(formula.is_empty(post_b)?);

	Ok(())
}

#[test]
fn identity_of_epsilon_contains_the_empty_pair() -> Result<(), Error> {
	let mut formula = Formula::new();
	let epsilon = formula.leaf(&epsilon_nfa())?;
	let diagonal = formula.identity(epsilon)?;
	let post = formula.post_image(epsilon, diagonal)?;
	let pre = formula.pre_image(epsilon, diagonal)?;

	assert!(!formula.is_empty(diagonal)?);
	assert!(!formula.is_empty(post)?);
	assert!(!formula.is_empty(pre)?);

	Ok(())
}

#[test]
fn identity_puts_one_alphabet_on_both_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let x = formula.leaf(&single(X))?;
	let diagonal = formula.identity(x)?;

	let options = Options::new()
		.tape_alphabet(0, *b"x")
		.tape_alphabet(1, *b"y");
	assert!(!formula.check(diagonal, &options)?.is_empty());

	Ok(())
}

#[test]
fn project_drops_inner_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let triple = formula.leaf(&tuple(&[A, B, C]))?;
	let projected = formula.project(triple, &[0, 2])?;
	let a = formula.leaf(&single(A))?;
	let c = formula.leaf(&single(C))?;
	let forward = formula.sync_product(a, projected, &[(0, 0)], &[Tape::Rhs(1)])?;
	let backward = formula.sync_product(c, projected, &[(0, 1)], &[Tape::Rhs(0)])?;

	assert_eq!(formula.arity(projected)?, 2);
	assert!(!formula.is_empty(projected)?);
	assert!(!formula.is_empty(forward)?);
	assert!(!formula.is_empty(backward)?);

	Ok(())
}

#[test]
fn project_is_existential() -> Result<(), Error> {
	let mut formula = Formula::new();
	let left = formula.leaf(&tuple(&[A, B, C]))?;
	let right = formula.leaf(&tuple(&[A, X, C]))?;
	let both = formula.union(left, right)?;
	let projected = formula.project(both, &[0, 2])?;
	let expected = formula.leaf(&tuple(&[A, C]))?;
	let unexpected = formula.leaf(&tuple(&[A, D]))?;
	let good = formula.intersection(projected, expected)?;
	let bad = formula.intersection(projected, unexpected)?;

	assert!(!formula.is_empty(good)?);
	assert!(formula.is_empty(bad)?);

	Ok(())
}

#[test]
fn project_to_zero_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let triple = formula.leaf(&tuple(&[A, B, C]))?;
	let empty = formula.leaf(&empty_nft())?;
	let some = formula.project(triple, &[])?;
	let none = formula.project(empty, &[])?;

	assert_eq!(formula.arity(some)?, 0);
	assert!(!formula.is_empty(some)?);
	assert!(formula.is_empty(none)?);

	Ok(())
}

#[test]
fn compose_on_keeps_unsynchronized_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let left = formula.leaf(&tuple(&[A, B, X]))?;
	let right = formula.leaf(&tuple(&[X, C, D]))?;
	let composed = formula.compose_on(left, right, &[(2, 0)])?;

	assert_eq!(formula.arity(composed)?, 4);
	assert!(!formula.is_empty(composed)?);

	Ok(())
}

#[test]
fn sync_product_to_zero_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let another_a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let same = formula.sync_product(a, another_a, &[(0, 0)], &[])?;
	let different = formula.sync_product(a, b, &[(0, 0)], &[])?;

	assert_eq!(formula.arity(same)?, 0);
	assert!(!formula.is_empty(same)?);
	assert!(formula.is_empty(different)?);

	Ok(())
}

#[test]
fn sync_product_keeps_the_concrete_symbol_matched_through_dont_care() -> Result<(), Error> {
	let mut formula = Formula::new();
	let left = formula.leaf(&tuple(&[A, DONT_CARE]))?;
	let right = formula.leaf(&tuple(&[B, C]))?;
	let synced = formula.sync_product(
		left,
		right,
		&[(1, 0)],
		&[Tape::Lhs(0), Tape::Lhs(1), Tape::Rhs(1)],
	)?;
	let expected = formula.leaf(&tuple(&[A, B, C]))?;
	let both = formula.intersection(synced, expected)?;

	assert_eq!(formula.arity(synced)?, 3);
	assert!(!formula.is_empty(both)?);

	let witness = formula
		.find_word(synced)?
		.expect("the product is not empty");
	assert_eq!(witness.to_string(), "(97, 98, 99)");

	Ok(())
}

// Sharing and rewrites

#[test]
fn shared_subterms() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let a_or_a = formula.union(a, a)?;
	let a_or_b = formula.union(a, b)?;
	let not_a_or_b = formula.complement(a_or_b)?;

	let cases = [
		(formula.intersection(a_or_a, a_or_a)?, false),
		(formula.intersection(not_a_or_b, not_a_or_b)?, false),
	];

	assert_emptiness(&formula, cases)
}

#[test]
fn self_operations() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let empty = formula.leaf(&empty_nfa())?;
	let a_star = formula.leaf(&star(A))?;
	let relation = formula.leaf(&tuple(&[A, B]))?;
	let a_or_a = formula.union(a, a)?;
	let a_star_or_a_star = formula.union(a_star, a_star)?;

	let cases = [
		(a_or_a, false),
		(formula.union(empty, empty)?, true),
		(formula.intersection(a, a)?, false),
		(formula.intersection(empty, empty)?, true),
		(formula.complement(a_star_or_a_star)?, true),
		(formula.intersection(relation, relation)?, false),
		(formula.union(relation, relation)?, false),
	];
	assert_emptiness(&formula, cases)?;

	let not_a_or_a = formula.complement(a_or_a)?;
	let not_a = formula.complement(a)?;
	assert_eq!(formula.is_empty(not_a_or_a)?, formula.is_empty(not_a)?);

	Ok(())
}

#[test]
fn identity_projections() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let empty = formula.leaf(&empty_nfa())?;
	let relation = formula.leaf(&tuple(&[A, B]))?;
	let empty_relation = formula.leaf(&empty_nft())?;
	let triple = formula.leaf(&tuple(&[A, B, C]))?;

	let cases = [
		(formula.project(a, &[0])?, false),
		(formula.project(empty, &[0])?, true),
		(formula.project(relation, &[0, 1])?, false),
		(formula.project(empty_relation, &[0, 1])?, true),
		(formula.project(triple, &[0, 1, 2])?, false),
	];

	assert_emptiness(&formula, cases)
}

#[test]
fn projections_of_identity() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let aa = formula.leaf(&tuple(&[A, A]))?;
	let ab = formula.leaf(&tuple(&[A, B]))?;
	let diagonal = formula.identity(a)?;

	for tape in [0, 1] {
		let back = formula.project(diagonal, &[tape])?;
		let with_a = formula.intersection(back, a)?;
		let with_b = formula.intersection(back, b)?;
		assert_eq!(formula.arity(back)?, 1);
		assert!(!formula.is_empty(back)?);
		assert!(!formula.is_empty(with_a)?);
		assert!(formula.is_empty(with_b)?);
	}

	for tapes in [[0, 1], [1, 0]] {
		let again = formula.project(diagonal, &tapes)?;
		let on = formula.intersection(again, aa)?;
		let off = formula.intersection(again, ab)?;
		assert_eq!(formula.arity(again)?, 2);
		assert!(!formula.is_empty(on)?);
		assert!(formula.is_empty(off)?);
	}

	let empty = formula.leaf(&empty_nfa())?;
	let empty_diagonal = formula.identity(empty)?;
	for tapes in [&[0][..], &[1], &[0, 1], &[1, 0]] {
		let projected = formula.project(empty_diagonal, tapes)?;
		assert!(formula.is_empty(projected)?);
	}

	Ok(())
}

#[test]
fn identity_fusion() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;
	let another_a = formula.leaf(&single(A))?;
	let b = formula.leaf(&single(B))?;
	let aa = formula.leaf(&tuple(&[A, A]))?;
	let ab = formula.leaf(&tuple(&[A, B]))?;
	let bb = formula.leaf(&tuple(&[B, B]))?;
	let id_a = formula.identity(a)?;
	let id_another_a = formula.identity(another_a)?;
	let id_b = formula.identity(b)?;

	let fused = formula.intersection(id_a, id_another_a)?;
	let on = formula.intersection(fused, aa)?;
	let off = formula.intersection(fused, ab)?;
	assert!(!formula.is_empty(fused)?);
	assert!(!formula.is_empty(on)?);
	assert!(formula.is_empty(off)?);

	let disjoint = formula.intersection(id_a, id_b)?;
	assert!(formula.is_empty(disjoint)?);

	let united = formula.union(id_a, id_b)?;
	let on_aa = formula.intersection(united, aa)?;
	let on_bb = formula.intersection(united, bb)?;
	let off_ab = formula.intersection(united, ab)?;
	assert!(!formula.is_empty(on_aa)?);
	assert!(!formula.is_empty(on_bb)?);
	assert!(formula.is_empty(off_ab)?);

	let self_fused = formula.intersection(id_a, id_a)?;
	let on = formula.intersection(self_fused, aa)?;
	let off = formula.intersection(self_fused, ab)?;
	assert!(!formula.is_empty(on)?);
	assert!(formula.is_empty(off)?);

	Ok(())
}

// Diagonal slices

/// `project(id(language) ∩ relation, [tape])`.
fn slice(
	formula: &mut Formula,
	language: Term,
	relation: Term,
	tape: usize,
) -> Result<Term, Error> {
	let id = formula.identity(language)?;
	let both = formula.intersection(id, relation)?;

	formula.project(both, &[tape])
}

#[test]
fn diagonal_slices() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a_star = formula.leaf(&star(A))?;
	let c_star = formula.leaf(&star(C))?;
	let empty = formula.leaf(&empty_nfa())?;
	let a = formula.leaf(&single(A))?;
	let diagonal = formula.leaf(&nft(2, 2, &[0], &[1], &[(0, &[A, A], 1), (0, &[B, B], 1)]))?;
	let off_diagonal = formula.leaf(&tuple(&[A, B]))?;
	let wildcard = formula.leaf(&tuple(&[DONT_CARE, A]))?;
	let slice_0 = slice(&mut formula, a_star, diagonal, 0)?;

	// The slice of tape 0 with the operands of the intersection swapped.
	let id_a_star = formula.identity(a_star)?;
	let swapped_operands = formula.intersection(diagonal, id_a_star)?;
	let swapped = formula.project(swapped_operands, &[0])?;

	let cases = [
		(slice_0, false),
		(slice(&mut formula, a_star, diagonal, 1)?, false),
		(swapped, false),
		(formula.intersection(slice_0, a)?, false),
		(slice(&mut formula, empty, diagonal, 0)?, true),
		(slice(&mut formula, a_star, off_diagonal, 0)?, true),
		(slice(&mut formula, c_star, diagonal, 0)?, true),
		(slice(&mut formula, a_star, wildcard, 0)?, false),
	];
	assert_emptiness(&formula, cases)?;

	let not_slice_0 = formula.complement(slice_0)?;
	let none = formula.intersection(not_slice_0, slice_0)?;
	assert!(!formula.is_empty(not_slice_0)?);
	assert!(formula.is_empty(none)?);

	Ok(())
}

// Contract

#[test]
fn arities() -> Result<(), Error> {
	let mut formula = Formula::new();
	let language = formula.leaf(&epsilon_nfa())?;
	let two = formula.leaf(&tuple(&[X, X]))?;
	let three = formula.leaf(&tuple(&[A, B, C]))?;
	let one_level = formula.leaf(&Nft::new(1)?)?;
	let id = formula.identity(language)?;
	let post = formula.post_image(language, two)?;
	let composed = formula.compose_on(three, three, &[(2, 0)])?;

	assert_eq!(formula.arity(one_level)?, 1);
	assert_eq!(formula.arity(three)?, 3);
	assert_eq!(formula.arity(id)?, 2);
	assert_eq!(formula.arity(post)?, 1);
	assert_eq!(formula.arity(composed)?, 4);

	Ok(())
}

#[test]
fn operations_reject_wrong_arities() -> Result<(), Error> {
	let mut formula = Formula::new();
	let language = formula.leaf(&epsilon_nfa())?;
	let two = formula.leaf(&tuple(&[X, X]))?;
	let three = formula.leaf(&tuple(&[A, B, C]))?;

	assert_eq!(
		formula.union(language, two),
		Err(Error::Arity {
			operation: "union",
			expected: 1,
			found: 2
		})
	);
	assert_eq!(
		formula.post_image(language, three),
		Err(Error::Arity {
			operation: "post_image",
			expected: 2,
			found: 3
		})
	);

	Ok(())
}

#[test]
fn project_rejects_bad_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let two = formula.leaf(&tuple(&[X, X]))?;

	assert_eq!(
		formula.project(two, &[0, 0]),
		Err(Error::RepeatedTape { tape: 0 })
	);
	assert_eq!(
		formula.project(two, &[2]),
		Err(Error::TapeOutOfRange { tape: 2, arity: 2 })
	);

	Ok(())
}

#[test]
fn sync_product_rejects_bad_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let two = formula.leaf(&tuple(&[X, X]))?;

	let out_of_range = formula.sync_product(two, two, &[(0, 2)], &[]);
	assert_eq!(
		out_of_range,
		Err(Error::SyncTapeOutOfRange {
			tape: Tape::Rhs(2),
			arity: 2
		})
	);

	let twice = formula.sync_product(two, two, &[(0, 0), (0, 1)], &[]);
	assert_eq!(twice, Err(Error::RepeatedSyncTape { tape: Tape::Lhs(0) }));

	let peer_twice = formula.sync_product(two, two, &[(0, 0)], &[Tape::Lhs(0), Tape::Rhs(0)]);
	assert_eq!(
		peer_twice,
		Err(Error::RepeatedSyncTape { tape: Tape::Rhs(0) })
	);

	Ok(())
}

#[test]
fn terms_belong_to_one_formula() -> Result<(), Error> {
	let mut formula = Formula::new();
	let mut other = Formula::new();
	let foreign = formula.leaf(&epsilon_nfa())?;

	assert_eq!(other.complement(foreign), Err(Error::ForeignTerm));
	assert_eq!(other.is_empty(foreign), Err(Error::ForeignTerm));

	let options = Options::new().explicit(foreign);
	let own = other.leaf(&epsilon_nfa())?;
	assert_eq!(other.check(own, &options), Err(Error::ForeignTerm));

	Ok(())
}

#[test]
fn query_alphabets_name_existing_tapes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a = formula.leaf(&single(A))?;

	let options = Options::new().tape_alphabet(1, *b"b");
	assert_eq!(
		formula.check(a, &options),
		Err(Error::TapeOutOfRange { tape: 1, arity: 1 })
	);

	Ok(())
}

#[test]
fn asymmetric_trees_keep_their_meaning() -> Result<(), Error> {
	let mut formula = Formula::new();
	let first_empty = formula.leaf(&empty_nfa())?;
	let second_empty = formula.leaf(&empty_nfa())?;
	let deep_empty = formula.union(first_empty, second_empty)?;
	let shallow_epsilon = formula.leaf(&epsilon_nfa())?;
	let left = formula.union(deep_empty, shallow_epsilon)?;
	let right = formula.union(shallow_epsilon, deep_empty)?;
	let not_left = formula.complement(left)?;
	let not_right = formula.complement(right)?;
	assert!(!formula.is_empty(left)?);
	assert!(!formula.is_empty(right)?);
	assert!(formula.is_empty(not_left)?);
	assert!(formula.is_empty(not_right)?);

	let big = formula.leaf(&nfa(8, &[0], &[0], &[]))?;
	let small = formula.leaf(&nfa(2, &[0], &[0], &[]))?;
	let big_and_small = formula.intersection(big, small)?;
	let small_and_big = formula.intersection(small, big)?;
	assert!(!formula.is_empty(big_and_small)?);
	assert!(!formula.is_empty(small_and_big)?);

	Ok(())
}
